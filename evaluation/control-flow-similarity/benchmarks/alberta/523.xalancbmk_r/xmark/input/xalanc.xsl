<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    version="1.0">
    
    <xsl:output encoding="utf-8"/>
    
    <xsl:template match="/"> 
        <out>
            <xsl:apply-templates/>           
        </out>
    </xsl:template> 
    
    <xsl:template match="/site">
        <!-- Q1 -->
        <xsl:for-each select="/site/open_auctions/open_auction">
            <xsl:copy-of select="bidder[1]/increase"/>                                    
        </xsl:for-each>
        
        <!-- Q2 -->
        <xsl:for-each select="people/person[@id='person0']">
            <xsl:copy-of select="name"/>                                    
        </xsl:for-each>
        
        <!-- Q3 -->
        <xsl:for-each select="open_auctions/open_auction">
            <xsl:if test="bidder[1]/increase *2 &lt;= bidder[last()]/increase">
                <increase first="{bidder[1]/increase}" last="{bidder[last()]/increase}"/>               
            </xsl:if>                                            
        </xsl:for-each>
        
        <!-- Q4 -->
        <xsl:for-each select="open_auctions/open_auction">
            <xsl:if test="bidder[personref/@person = 'person16']/following-sibling::bidder[personref/@person='person250']">
                <history>
                    <xsl:value-of select="reserve"/>
                </history>               
            </xsl:if>                                            
        </xsl:for-each>
        
        <!-- Q5 -->
        <xsl:value-of select="count(closed_auctions/closed_auction/price[. >= 40])"/>
        
        <!-- Q6 -->
        <xsl:for-each select="regions/*">
            <itemCount>
                <xsl:value-of select="count(.//item)"/>  
            </itemCount>
        </xsl:for-each> 
        
        <!-- Q7 -->
        <xsl:value-of select="count(//description) + count(//annotation) + count(//email)"/>    
        
        <!-- Q8 -->
        <xsl:for-each select="people/person">
            <xsl:variable name="a" 
                select="/site/closed_auctions/closed_auction[buyer/@person = current()/@id]"/>
            <item person="{name}">
                <xsl:value-of select="count($a)"/>
            </item>  
        </xsl:for-each>
        
        <!-- Q9 and Q10 not compatible -->
        <!-- Q11 -->
        <xsl:variable name="p" select="people/person"/>
        <xsl:for-each select="$p">
            <xsl:variable name="i" select="/site/open_auctions/open_auction/initial[current()/profile/@income > (5000 * .)]"/>
            <items name="{current()/name}">
                <xsl:value-of select="count($i)"/>
            </items>
        </xsl:for-each>
        
        <!-- Q12 -->
        <xsl:variable name="q" select="people/person[profile/@income > 50000]"/>
        <xsl:for-each select="$q">
            <xsl:variable name="i" select="/site/open_auctions/open_auction/initial[current()/profile/@income > (5000 * .)]"/>
            <items name="{current()/name}">
                <xsl:value-of select="count($i)"/>
            </items>
        </xsl:for-each>
        
        <!-- Q13 -->
        <xsl:for-each select="regions/australia/item">            
            <items name="{current()/name}">
                <xsl:copy-of select="current()/description"/>
            </items>
        </xsl:for-each>
        
        <!-- Q14 -->
        <xsl:for-each select="//item[contains(./description,'gold')]">
            <xsl:copy-of select="current()/name"/>
            <xsl:copy-of select="current()/description"/>            
        </xsl:for-each>
        
        <!-- Q15 -->
        <xsl:for-each select="closed_auctions/closed_auction/annotation/description/parlist/listitem/parlist/listitem/text/emph/keyword">            
            <text>
                <xsl:copy-of select="."/>
            </text>
        </xsl:for-each>
        
        <!-- Q16 -->
        <xsl:for-each select="closed_auctions/closed_auction[annotation/description/parlist/listitem/parlist/listitem/text/emph/keyword]">            
            <person id="{./seller/@person}"/>                
        </xsl:for-each>
        
        <!-- Q17 -->
        <xsl:for-each select="people/person[not(homepage/text())]"> 
            <person name="{./name}"/>               
        </xsl:for-each>
        
        <!-- Q18 -->
        <xsl:for-each select="open_auctions/open_auction/reserve"> 
            <reserve>
                <xsl:value-of select="(.) * 2.20371"/> <!-- Parentheses added to avoid XT driver bug -->
            </reserve>
        </xsl:for-each>
        
        <!-- Q19 -->
        <xsl:for-each select="regions//item">
            <xsl:sort select="./name"/>
            <item name="{./name}">
                <xsl:copy-of select="./location"/>
            </item>
        </xsl:for-each>
        
        <!-- Q20 -->
        <result>
            <preferred>
                <xsl:value-of select="count(people/person/profile[@income &gt;= 100000])"/>
            </preferred>
            <standard>
                <xsl:value-of select="count(people/person/profile[@income &lt; 100000 and @income &gt;= 30000])"/>
            </standard>
            <challenge>
                <xsl:value-of select="count(people/person/profile[@income &lt; 30000])"/>
            </challenge>
            <na>
                <xsl:value-of select="count(people/person[not(profile/@income)])"/>
            </na>
        </result>
    </xsl:template>
    
</xsl:stylesheet>
